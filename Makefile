#make
XPDF_BASE = ./xpdf-4.05
DEFS = -DWIN32 -D_WIN32 -DCONSOLE -DNDEBUG -D_WIN32_WINNT=0x0501 -DWINVER=0x0501 -DNTDDI_VERSION=0x05010000 -D_WIN32_IE=0x0800 -DWIN32_LEAN_AND_MEAN -DMINGW_HAS_SECURE_API -DSTRSAFE_NO_DEPRECATE -D__MINGW_USE_VC2005_COMPAT 
INCLUDE=-I$(XPDF_BASE) -I$(XPDF_BASE)/fofi -I$(XPDF_BASE)/xpdf -I$(XPDF_BASE)/goo -I$(XPDF_BASE)/splash -I./common -I.
WARNINGS = -Wall -Wextra -Wno-format -Wno-missing-braces -Wno-unknown-pragmas -Wno-missing-field-initializers -Wno-unused-parameter -Wno-multichar
CFLAGS = $(DEFS) $(INCLUDE) -O2 -static -fno-strict-aliasing $(WARNINGS) -mms-bitfields -fms-extensions -municode
CXXFLAGS = $(CFLAGS) -fno-exceptions
CC = gcc
CXX = g++ -std=c++17
RM = rm -rf
EXEEXT = .wdx
LINK = $(CXX) -mwindows -mdll
LDFLAGS = -Wl,--dynamicbase,--nxcompat,--kill-at,--major-os-version=5,--minor-os-version=1,--major-subsystem-version=5,--minor-subsystem-version=1 -flto=4 -fuse-linker-plugin -static-libgcc -static-libstdc++
LIBS = -lole32
VPATH= $(XPDF_BASE)/fofi:$(XPDF_BASE)/goo:$(XPDF_BASE)/xpdf
SRC_CC = FoFiBase.cc FoFiEncodings.cc FoFiIdentifier.cc FoFiTrueType.cc FoFiType1.cc FoFiType1C.cc \
        gfile.cc GHash.cc GList.cc gmem.cc GString.cc \
        AcroForm.cc Annot.cc Array.cc BuiltinFont.cc BuiltinFontTables.cc Catalog.cc CharCodeToUnicode.cc CMap.cc \
        Decrypt.cc Dict.cc Error.cc FontEncodingTables.cc Function.cc Gfx.cc GfxFont.cc \
        GfxState.cc GlobalParams.cc Lexer.cc Link.cc NameToCharCode.cc Object.cc \
        OptionalContent.cc Outline.cc OutputDev.cc Page.cc Parser.cc PDFDoc.cc PDFDocEncoding.cc PSTokenizer.cc \
        SecurityHandler.cc Stream.cc TextOutputDev.cc TextString.cc UnicodeMap.cc UnicodeRemapping.cc UnicodeTypeTable.cc \
        UTF8.cc XRef.cc XFAScanner.cc Zoox.cc \
        ThreadData.cc PDFDocEx.cc PDFExtractor.cc TcOutputDev.cc xPDFInfo.cc
SRC_RC= xPDFSearch.rc

.SUFFIXES: .o .obj .c .cpp .cxx .cc .h .hh .hxx $(EXEEXT) .rc .res

all: xPDFSearch$(EXEEXT)

OBJS_CC = $(SRC_CC:.cc=.o)
OBJS_RC = $(SRC_RC:.rc=.o)

.rc.o:
	windres --codepage=65001 --output-format=coff --input-format=rc --input=$< --output=$@
	
.cc.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

xPDFSearch$(EXEEXT): $(OBJS_CC) $(OBJS_RC)
	$(LINK) $(LDFLAGS) -o $@ $(OBJS_CC) $(OBJS_RC) $(LIBS)

clean:
	-$(RM) *.obj
	-$(RM) *.o
	-$(RM) *.res
	-$(RM) xPDFSearch$(EXEEXT)

