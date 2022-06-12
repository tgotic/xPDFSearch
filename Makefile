#make
DEFS = -DWIN32 -D_WIN32 -DCONSOLE -DNDEBUG -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 -DNTDDI_VERSION=0x06010000 -D_WIN32_IE=0x0800 -DWIN32_LEAN_AND_MEAN -DMINGW_HAS_SECURE_API -DSTRSAFE_NO_DEPRECATE -D__MINGW_USE_VC2005_COMPAT 
INCLUDES=-I./xpdf-4.04 -I./xpdf-4.04/fofi -I./xpdf-4.04/xpdf -I./xpdf-4.04/goo -I./xpdf-4.04/splash -I./xpdf-4.04 -I./common -I.
WARNINGS = -Wall -Wextra -Wno-format -Wno-missing-braces -Wno-unknown-pragmas -Wno-missing-field-initializers -Wno-unused-parameter -Wno-multichar
CFLAGS = $(DEFS) $(INCLUDES) -O2 -static -fno-strict-aliasing $(WARNINGS) -mms-bitfields -fms-extensions -municode -std=c++11
CXXFLAGS = $(CFLAGS) -fno-exceptions
CC = gcc 
CXX = g++
RM = rm -rf
EXEEXT = .wdx
LINK = g++.exe -std=c++11 -mwindows -municode -mdll -static
LDFLAGS = -Wl,--dynamicbase,--nxcompat,--kill-at,--major-os-version=6,--minor-os-version=1,--major-subsystem-version=6,--minor-subsystem-version=1 -flto=4 -fuse-linker-plugin -static-libgcc -static-libstdc++
LIBS = 
VPATH= ./xpdf-4.04/fofi:./xpdf-4.04/goo:./xpdf-4.04/xpdf
SRCCXX = FoFiBase.cc FoFiEncodings.cc FoFiIdentifier.cc FoFiTrueType.cc FoFiType1.cc FoFiType1C.cc \
        gfile.cc GHash.cc GList.cc gmem.cc GString.cc \
        AcroForm.cc Annot.cc Array.cc BuiltinFont.cc BuiltinFontTables.cc Catalog.cc CharCodeToUnicode.cc CMap.cc \
        Decrypt.cc Dict.cc Error.cc FontEncodingTables.cc Form.cc Function.cc Gfx.cc GfxFont.cc \
        GfxState.cc GlobalParams.cc JArithmeticDecoder.cc Lexer.cc Link.cc NameToCharCode.cc Object.cc \
        OptionalContent.cc OutputDev.cc Page.cc Parser.cc PDFDoc.cc PDFDocEncoding.cc PSTokenizer.cc \
        SecurityHandler.cc Stream.cc TextOutputDev.cc TextString.cc UnicodeMap.cc UnicodeRemapping.cc UnicodeTypeTable.cc \
        UTF8.cc XFAForm.cc XRef.cc Zoox.cc \
        PDFExtractor.cc TcOutputDev.cc xPDFInfo.cc
SRCRES= xPDFSearch.rc

.SUFFIXES: .o .obj .c .cpp .cxx .cc .h .hh .hxx $(EXEEXT) .rc .res

all: xPDFSearch.wdx

OBJSC = $(SRCC:.c=.o)
OBJSCXX = $(SRCCXX:.cc=.o)
OBJSRES = $(SRCRES:.rc=.res)

.rc.res:
	windres --codepage=65001 --output-format=coff --input-format=rc --input=$< --output=$@
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

.cpp.o:
	$(CXX) $(CXXFLAGS) -c -o $@ $<

xPDFSearch.wdx: $(OBJSC) $(OBJSCXX) $(OBJSRES)
	$(LINK) $(LDFLAGS) -o $@ $(OBJSC) $(OBJSCXX) $(OBJSRES) $(LIBS)

clean:
	-$(RM) *.obj
	-$(RM) *.o
	-$(RM) *.res
	-$(RM) xPDFSearch.wdx
	
  
