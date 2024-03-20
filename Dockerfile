# Use the official Apache HTTP Server image as the base
FROM debian:bookworm

ENV DEBIAN_FRONTEND=noninteractive

# Maps the HTTP port of the container to a port of the host
EXPOSE 8081:80

# Install development tools
RUN apt-get update && \
    apt-get install -y \
	apache2 \
	apache2-dev \
	libapr1-dev \
	libaprutil1-dev\
	build-essential \
	libjson-c-dev \
	vim \
	&& rm -rf /var/lib/apt/lists/*

ENV DEBIAN_FRONTEND=dialog

# Update https.conf so Apache loads the positions.conf that loads the compiled module

RUN bash -c 'echo -e "LoadModule positions_module /usr/lib/apache2/modules/mod_positions.so" > /etc/apache2/mods-available/positions.load'
RUN bash -c 'echo -e "LoadModule italiancaps_module /usr/lib/apache2/modules/mod_italiancaps.so" > /etc/apache2/mods-available/italiancaps.load'
RUN bash -c 'echo -e "LoadModule tree_module /usr/lib/apache2/modules/mod_tree.so" > /etc/apache2/mods-available/tree.load'
RUN bash -c 'echo -e "<Location /positions>\n  SetHandler positions\n</Location>\n" > /etc/apache2/mods-available/positions.conf'
RUN bash -c 'echo -e "<Location /italiancaps>\n  SetHandler italiancaps\n</Location>\n" > /etc/apache2/mods-available/italiancaps.conf' 
RUN bash -c 'echo -e "<Location /tree>\n  SetHandler tree\n</Location>\n" > /etc/apache2/mods-available/tree.conf'
RUN bash -c 'ln -s /etc/apache2/mods-available/positions.load /etc/apache2/mods-enabled/'
RUN bash -c 'ln -s /etc/apache2/mods-available/italiancaps.load /etc/apache2/mods-enabled/'
RUN bash -c 'ln -s /etc/apache2/mods-available/tree.load /etc/apache2/mods-enabled/'
RUN bash -c 'ln -s /etc/apache2/mods-available/positions.conf /etc/apache2/mods-enabled/'
RUN bash -c 'ln -s /etc/apache2/mods-available/italiancaps.conf /etc/apache2/mods-enabled/'
RUN bash -c 'ln -s /etc/apache2/mods-available/tree.conf /etc/apache2/mods-enabled/'

# Prepare the directories
RUN mkdir -p /tree/italy/
RUN mkdir -p /tree/italy/emilia-romagna
RUN mkdir -p /tree/italy/emilia-romagna/raravenna/russi/48026
RUN mkdir -p /tree/italy/trentino
RUN mkdir -p /cap
RUN ln -s /tree/italy/emilia-romagna/raravenna/russi /cap/48026

COPY ./index.html /var/www/html

# Set the working directory
WORKDIR /usr/src/apache_module

# Copy your Apache module source code into the container
COPY ./config.h .
COPY ./mod_positions.c .
COPY ./mod_italiancaps.c .
COPY ./mod_tree.c .

# Compile and install your Apache module -i install -c compile -n modulename
RUN apxs -i -c -n positions mod_positions.c
RUN apxs -i -c -n italiancaps mod_italiancaps.c
RUN apxs -i -c -n tree mod_tree.c -Wc,-I/usr/include/json-c -Wl, -ljson-c

# Start Apache
CMD ["apachectl", "-D", "FOREGROUND"]

