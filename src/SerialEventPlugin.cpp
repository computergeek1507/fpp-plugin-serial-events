#include <fpp-pch.h>

#include <unistd.h>
#include <ifaddrs.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <cstring>
#include <fstream>
#include <list>
#include <vector>
#include <sstream>
#include <httpserver.hpp>
#include <cmath>
#include <mutex>
#include <regex>
#include <thread>
#include <chrono>
#include <future>
#include <atomic>

#include <termios.h>

#include "commands/Commands.h"
#include "common.h"
#include "settings.h"
#include "Plugin.h"
#include "log.h"

#include "channeloutput/serialutil.h"

struct SerialCondition {
    SerialCondition() {}
    explicit SerialCondition(Json::Value &v) {
        conditionType = v["condition"].asString();
        val = v["conditionValue"].asString();        
    }
    
    bool matches(std::string const& ev) {
        if (conditionType == "contains") {
            return ev.find(val) != std::string::npos;
        } else if (conditionType == "startswith") {
            return ev.starts_with(val);
        } else if (conditionType == "endswith") {
            return ev.ends_with(val);
        } else if (conditionType == "regex") {
            try{
                std::regex self_regex(val, std::regex_constants::ECMAScript | std::regex_constants::icase);
                return std::regex_search(ev, self_regex);
            } catch(std::exception &ex) {
                LogErr(VB_PLUGIN, "Regex Error '%s'\n", ex.what());
                return false;
            }
        } 
        return false;
    }    
    std::string conditionType;
    std::string val;
};

struct SerialCommandArg {
    explicit SerialCommandArg(const std::string &t) : arg(t) { }
    ~SerialCommandArg() { }    
    std::string arg;
    std::string type;
};


struct SerialEvent {
    explicit SerialEvent(Json::Value &v) {
        description = v["description"].asString();
        condition = SerialCondition(v);

        command = v;
        command.removeMember("argTypes");
        command.removeMember("args");
        command.removeMember("condition");
        command.removeMember("conditionValue");
        command.removeMember("description");

        if (v.isMember("args")) {
            for (int x = 0; x < v["args"].size(); x++) {
                args.push_back(SerialCommandArg(v["args"][x].asString()));
            }
        }
        if (v.isMember("argTypes")) {
            for (int x = 0; x < v["argTypes"].size(); x++) {
                args[x].type = v["argTypes"][x].asString();
            }
        }
    }
    ~SerialEvent() {
        args.clear();
    }
    
    bool matches(std::string const& ev) {
        return condition.matches(ev);
    }
    
    void invoke(std::string &ev) {       
        Json::Value newCommand = command;
        for (auto &a : args) {
            std::string tp = "string";
            if (a.type == "bool" || a.type == "int") {
                tp = a.type;
            }
            
            //printf("Eval p: %s\n", a.arg.c_str());
            std::string r = a.arg;

            if(tp == "string") {
                if(r.find("%VAL%") != std::string::npos) {
                    replaceAll(r, "%VAL%" , ev);
                }
            }
            //printf("        -> %s\n", r.c_str());
            newCommand["args"].append(r);
        }

        CommandManager::INSTANCE.run(newCommand);
    }
    

    std::string description;    
    SerialCondition condition;
    
    Json::Value command;
    std::vector<SerialCommandArg> args;
};

class SerialEventPlugin : public FPPPlugin, public httpserver::http_resource {
public:
    std::vector<std::unique_ptr<SerialEvent>> serial_events;
    std::list<std::string> serial_data;
    
    int m_fd {-1};
    bool enabled {false};
  
    SerialEventPlugin() : FPPPlugin("fpp-plugin-serial_event") {
        LogInfo(VB_PLUGIN, "Initializing Serial Event Plugin\n");        
        enabled = InitSerial();
    }
    virtual ~SerialEventPlugin() {   
        CloseSerial();     
    }
    bool InitSerial() {
        if (FileExists(FPP_DIR_CONFIG("/plugin.serial-event.json"))) {
            std::string port;
            int speed = 115200;
            try {
                Json::Value root;
                bool success =  LoadJsonFromFile(FPP_DIR_CONFIG("/plugin.serial-event.json"), root);
                if (root.isMember("serialEvents")) {
                    for (int x = 0; x < root["serialEvents"].size(); x++) {
                        serial_events.emplace_back(std::make_unique<SerialEvent>(root["serialEvents"][x]));
                    }
                }

                if (root.isMember("port")) {
                    port = root["port"].asString();
                }
                if (root.isMember("speed")) {
                    speed = root["speed"].asInt();
                }  
                LogInfo(VB_PLUGIN, "Using %s Serial Output Speed %d\n", port.c_str(), speed);
                if(port.empty()) {
                    LogErr(VB_PLUGIN, "Serial Port is empty '%s'\n", port.c_str());
                    return false;
                }
                if(port.find("/dev/") == std::string::npos)
                {
                    port = "/dev/" + port;
                }
                int fd = SerialOpen(port.c_str(), speed, "8N1", false);
                if (fd < 0) {
                    LogErr(VB_PLUGIN, "Could Not Open Serial Port '%s'\n", port.c_str());
                    return false;
                }
                m_fd = fd;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                tcflush(m_fd,TCIOFLUSH);
                ioctl(m_fd, TCIOFLUSH, 2); 
                LogInfo(VB_PLUGIN, "Serial Input Started\n");
                return true;
            } catch (...) {
                LogErr(VB_PLUGIN, "Could not Initialize Serial Port '%s'\n", port.c_str());
            }                
        }else{
            LogInfo(VB_PLUGIN, "No plugin.serial-event.json config file found\n");
        }
        return false;
    }

    void CloseSerial() {
        if (m_fd >= 0) {
            SerialClose(m_fd);
            m_fd = -1;
        }
    }

    int SerialDataAvailable(int fd) {
        int bytes {0};
        ioctl(fd, FIONREAD, &bytes);
        return bytes;
    }

    int SerialDataRead(int fd, char* buf, size_t len) {
        // Read() (using read() ) will return an 'error' EAGAIN as it is
        // set to non-blocking. This is not a true error within the
        // functionality of Read, and thus should be handled by the caller.
        int n = read(fd, buf, len);
        if((n < 0) && (errno == EAGAIN)) return 0;
        return n;
    }

    void remove_control_characters(std::string& s) {
        s.erase(std::remove_if(s.begin(), s.end(), [](char c) { return std::iscntrl(c); }), s.end());
    }
    virtual const std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request &req) override {
        std::string v;
        
        if (req.get_path_pieces().size() > 1) {
            std::string p1 = req.get_path_pieces()[1];
            if (p1 == "list") {
                for (auto &sd : serial_data) {
                    v += sd + "\n";
                }
                return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(v, 200));
            } 
        }
        return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Not Found", 404));
    }
    void registerApis(httpserver::webserver *m_ws) override {
        m_ws->register_resource("/SERIALEVENT", this, true);
    }

     virtual void addControlCallbacks(std::map<int, std::function<bool(int)>> &callbacks) {
        if (enabled) {
            std::function<bool(int)> fn = [this](int d) {
                char buffer[100];
                while(1) {
                    ssize_t length = read(m_fd, &buffer, sizeof(buffer));
                    if (length == -1) {
                        break;
                    } else if (length == 0) {
                        break;
                    } else {
                        buffer[length] = '\0';
                    }
                }

                std::string serialData = buffer;

                remove_control_characters(serialData);
                if(!serialData.empty()) {
                    LogInfo(VB_PLUGIN, "Serial data found '%s'\n", serialData.c_str());
                    serial_data.push_back(serialData);
                    if (serial_data.size() > 25) {
                        serial_data.pop_front();
                    }
                    for (auto &a : serial_events) {
                        if (a->matches(serialData)) {
                            a->invoke(serialData);
                        }
                    }
                }
 
            return false;
        };
        callbacks[m_fd] = fn;
        }
    }
};


extern "C" {
    FPPPlugin *createPlugin() {
        return new SerialEventPlugin();
    }
}
