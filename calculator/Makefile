GCC=nspire-gcc
LD=nspire-ld
GENZEHN=genzehn
GCCFLAGS=-Wall -Wextra -Wno-unused-parameter -marm -Os
LDFLAGS=-Wl,--gc-sections
ZEHNFLAGS=--name "NspireAI" --author "NspireAI" --notice "Gemini USB frontend" --ndless-min 36 --240x320-support true
OBJS=main.o
DISTDIR?=../dist
all: $(DISTDIR)/nspire_ai.tns
main.o: main.c
	$(GCC) $(GCCFLAGS) -c $< -o $@
$(DISTDIR)/nspire_ai.elf: $(OBJS)
	mkdir -p $(DISTDIR)
	$(LD) $^ -o $@ $(LDFLAGS)
$(DISTDIR)/nspire_ai.tns: $(DISTDIR)/nspire_ai.elf
	$(GENZEHN) --input $< --output $@.zehn $(ZEHNFLAGS)
	make-prg $@.zehn $@
	rm -f $@.zehn
clean:
	rm -f $(OBJS) $(DISTDIR)/nspire_ai.elf $(DISTDIR)/nspire_ai.tns
.PHONY: all clean
