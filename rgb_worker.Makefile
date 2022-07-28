include Makefile.defs

TARGET = rgb_worker

CXXSRCS = \
	mdp/MutualHeartbeatMonitor.cpp \
	mdp/Worker.cpp \
	mdp/WorkerTask.cpp \
	mdp/ZMQIdentity.cpp \
	mdp/ZMQWorkerContext.cpp \
	rgb_worker.cpp

include Makefile.rules
