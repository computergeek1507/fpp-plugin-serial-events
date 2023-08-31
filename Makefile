include /opt/fpp/src/makefiles/common/setup.mk

all: libfpp-plugin-serial-event.so
debug: all

OBJECTS_fpp_serial_event_so += src/SerialEventPlugin.o
LIBS_fpp_serial_event_so += -L/opt/fpp/src -lfpp
CXXFLAGS_src/SerialEventPlugin.o += -I/opt/fpp/src

%.o: %.cpp Makefile
	$(CCACHE) $(CC) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@

libfpp-plugin-serial-event.so: $(OBJECTS_fpp_serial_event_so) /opt/fpp/src/libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_serial_event_so) $(LIBS_fpp_serial_event_so) $(LDFLAGS) -o $@

clean:
	rm -f libfpp-plugin-serial-event.so $(OBJECTS_fpp_serial_event_so)


