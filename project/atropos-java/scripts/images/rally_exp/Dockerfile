FROM ubuntu:20.04

ARG UNAME=rally
ARG UID=1000

RUN apt update
RUN apt install -y python3-pip git
RUN pip install esrally

RUN groupadd -g $UID -o $UNAME
RUN useradd -m -u $UID -g $UID -o -s /bin/bash $UNAME

USER $UNAME
WORKDIR /home/$UNAME

CMD /bin/bash
