# Use the official Apache HTTP Server image as the base
FROM httpd:latest

# Maps the HTTP port of the container to a port of the host
EXPOSE 8081:80

# Install development tools
RUN apt-get update && \
    apt-get install -y gcc \
                       make \
                       libc-dev \
                       apache2-dev

# Update https.conf so Apache loads the positions.conf that loads the compiled module
# RUN bash -c 'echo -e "\nLoadModule positions_module modules/mod_positions.so\n<Location /positions>\n  SetHandler positions\n</Location>\n" >> /usr/local/apache2/conf/httpd.conf'

RUN bash -c 'echo -e "\nInclude conf/extra/positions.conf\n" >> /usr/local/apache2/conf/httpd.conf'
COPY ./positions.conf /usr/local/apache2/conf/extra

# Prepare the directories
RUN mkdir -p /countries/regions/

# Set the working directory
WORKDIR /usr/src/apache_module

# Copy your Apache module source code into the container
COPY ./mod_positions.c .

# Compile and install your Apache module -i install -c compile -n modulename
RUN apxs -i -c -n positions mod_positions.c

# Start Apache
CMD ["httpd", "-D", "FOREGROUND"]

