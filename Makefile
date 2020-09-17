std: std.c compile.sh
	./compile.sh std

tcp: tcp.c compile.sh
	./compile.sh tcp

tls: tls.c compile.sh
	./compile.sh tls

clean: clean.sh
	./clean.sh
