FROM ubuntu:18.04
ENV TERM linux
## for apt to be noninteractive
ENV DEBIAN_FRONTEND noninteractive
ENV DEBCONF_NONINTERACTIVE_SEEN true

RUN echo "travis_fold:start:Dapt\033[33;1mservice Dockerfile apt\033[0m"

RUN apt-get update && \
    apt-get install -y wget bsdmainutils python3 htop strace rsyslog supervisor netcat htop jq nginx

COPY setup/nginx.conf /etc/nginx/nginx.conf
COPY setup/default /etc/nginx/sites-available/default

#RUN apt-get install -y vim gdb screen
RUN echo "\ntravis_fold:end:Dapt\r"

RUN useradd -s /bin/bash -m oooser

RUN mkdir /gol ; chown oooser:oooser /gol

COPY --chown=oooser:www-data setup/.golly/Rules/Varlife.rule /gol/Varlife.rule
COPY --chown=oooser:www-data setup/supervisord.conf /etc/supervisord.conf

COPY --chown=oooser:www-data src/play_blackjack.py           \
                             src/laas-bj-dealer-256byte.mc   \
                             src/laas-bj-shuffler_v3.mc      \
                             src/reseter.py                  \
                             src/little_bot.py               \
                             /gol/

COPY --chown=oooser:www-data src/bj /gol/bj

COPY --chown=oooser:www-data src/bgolly-dealer /gol
COPY --chown=oooser:www-data src/bgolly-controller /gol

RUN chmod +x /gol/play_blackjack.py /gol/bgolly-*

USER oooser
RUN mkdir /tmp/inputs /gol/bj/results

##ADDHERE

COPY --chown=oooser:www-data src/state.dat /gol/state.dat
RUN mkdir -p /gol/inputs
COPY src/laas-bj-teams-controller-1.mc /tmp/inputs/laas-bj-teams-controller-1.mc
COPY src/laas-bj-teams-controller-1.mc /tmp/inputs/laas-bj-teams-controller-2.mc
COPY src/laas-bj-teams-controller-1.mc /tmp/inputs/laas-bj-teams-controller-3.mc
USER root

CMD /etc/init.d/nginx start && /usr/bin/supervisord
