LIBPATH =../../
BOARD=vc707
BUILDTOOLS=$(LIBPATH)/buildtools/

BLIBPATH=$(LIBPATH)/../bluelib/src/

CUSTOMBSV= -p +:$(LIBPATH)/src:$(BLIBPATH)/:$(BLIBPATH)/msfp
CUSTOMCPP_BSIM= $(BLIBPATH)/bdpi.cpp

include $(BUILDTOOLS)/Makefile.base
