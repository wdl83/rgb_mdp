include Makefile.defs

CFLAGS += $(DEFS)
CXXFLAGS += $(DEFS) 

TARGET = rgb_worker

# CSRCS =

CXXSRCS = \
	../mdp/Worker.cpp \
	../mdp/MutualHeartbeatMonitor.cpp \
	../mdp/Worker.cpp \
	../mdp/WorkerTask.cpp \
	../mdp/ZMQIdentity.cpp \
	../mdp/ZMQWorkerContext.cpp \
	rgb_worker.cpp

include Makefile.rules
