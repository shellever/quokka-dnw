all: dnw

dnw: dnw.c
	gcc -o $@ $^ -lusb

install:
	sudo cp -fp 51-dnw.rules /etc/udev/rules.d/
	sudo service udev restart

clean:
	rm -f dnw

.PHONY: all clean

