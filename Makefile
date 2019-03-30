all: dnw2

dnw2: dnw2.c
	gcc -o $@ $^ -lusb

install:
	sudo cp -fp 51-dnw.rules /etc/udev/rules.d/
	sudo service udev restart

clean:
	rm -f dnw2 

.PHONY: all clean

