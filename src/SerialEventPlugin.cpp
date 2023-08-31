#include <fpp-pch.h>

#include <unistd.h>
#include <ifaddrs.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <list>
#include <vector>
#include <sstream>
#include <httpserver.hpp>
#include <cmath>
#include <mutex>
#include <regex>

#include "commands/Commands.h"
#include "common.h"
#include "settings.h"
#include "Plugin.h"
#include "log.h"

struct SerialCondition {
    SerialCondition() {}
    explicit SerialCondition(Json::Value &v) {
        conditionType = v["condition"].asString();
        val = v["conditionText"].asString();        
    }
    
    bool matches(std::string const& ev) {
        if (conditionType == "contains") {
            return ev.find(val) != std::string::npos;
        } else if (conditionType == "startswith") {
            return ev.starts_with(val);
        } else if (conditionType == "endswith") {
            return ev.ends_with(val);
        } else if (conditionType == "regex") {
            std::regex self_regex(val, std::regex_constants::ECMAScript | std::regex_constants::icase);
            return std::regex_search(ev, self_regex);
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
        path = v["path"].asString();
        description = v["description"].asString();        
        condition = SerialCondition(v["condition"]);        

        command = v;
        command.removeMember("path");
        command.removeMember("argTypes");
        command.removeMember("args");
        command.removeMember("conditions");
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
    
    std::string path;
    std::string description;
    
    SerialCondition condition;
    
    Json::Value command;
    std::vector<SerialCommandArg> args;
};

class SerialEventPlugin : public FPPPlugin, public httpserver::http_resource {
public:
    std::vector<std::unique_ptr<SerialEvent>> serial_events;
    std::vector<std::string> serial_data;
    
    std::mutex queueLock;
  
    SerialEventPlugin() : FPPPlugin("fpp-plugin-serial_event") {
        LogInfo(VB_PLUGIN, "Initializing Serial Event Plugin\n");

        
        if (FileExists(FPP_DIR_CONFIG("/plugin.serial-event.json"))) {
            Json::Value root;
            bool success =  LoadJsonFromFile(FPP_DIR_CONFIG("/plugin.serial-event.json"), root);
            if (root.isMember("events")) {
                for (int x = 0; x < root["events"].size(); x++) {
                    serial_events.emplace_back(std::make_unique<SerialEvent>(root["events"][x]));
                }
            }
            if (root.isMember("ports")) {
                for (int x = 0; x < root["ports"].size(); x++) {
                    if (root["ports"][x]["enabled"].asBool()) {
                        std::string name = root["ports"][x]["name"].asString();
                        try {
                           
                        } catch (...) {
                            LogErr(VB_PLUGIN, "Could not initialize Serial Event plugin for port %s\n", name.c_str());
                        }
                    }
                }
            }
        }
    }
    virtual ~SerialEventPlugin() {        
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
};


extern "C" {
    FPPPlugin *createPlugin() {
        return new SerialEventPlugin();
    }
}
