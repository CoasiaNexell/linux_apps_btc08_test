btc08_test_CPPFLAGS = $(PTHREAD_FLAGS)
btc08_test_LDFLAGS = $(PTHREAD_FLAGS)
btc08_test_LDADD = @PTHREAD_LIBS@

btc08_test_CPPFLAGS += -I. -I./engine

bin_PROGRAMS = btc08_test
btc08_test_SOURCES = \
	engine/NX_DbgMsg.c \
	engine/TempCtrl.c \
	engine/GpioControl.c \
	engine/Spi.c \
	engine/Btc08.c \
	engine/Utils.c \
	engine/NX_Queue.c \
	engine/sha2.c \
	SingleCommand.c \
	SimpleWork.c \
	ScenarioTest.c \
	TestVector.c \
	main.c
