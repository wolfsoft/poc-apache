# POC Apache module build and execution container
#

FROM debian:buster

WORKDIR /root

COPY ["mod_poc.c", "mod_poc.c"]
COPY ["index.html", "/var/www/html/"]
COPY ["test.php", "/var/www/html/cgi/"]
COPY ["test.php", "/var/www/html/fastcgi/"]
COPY ["000-default.conf", "/etc/apache2/sites-enabled/"]

RUN export DEBIAN_FRONTEND=noninteractive \
	&& apt-get update \
	&& apt-get -y install apache2 apache2-dev libapache2-mod-fcgid php-cgi \
	&& a2enmod cgid fcgid actions

RUN apxs2 -i -a -c mod_poc.c

EXPOSE 80
CMD apachectl -D FOREGROUND
