FROM fedora:37 AS build-ocpdoom
WORKDIR /go/src/ocpvizdoom
ADD go.mod .
ADD ocpvizdoom.go .
RUN dnf install golang -y &&  dnf clean all
RUN CGO_ENABLED=0 GOOS=linux go build -a -installsuffix cgo -o ocpvizdoom .

FROM fedora:37 AS build-essentials
ARG TARGETARCH=amd64
ARG KUBECTL_VERSION=1.25.4
RUN dnf update -y && dnf install wget ca-certificates -y
RUN echo "TARGETARCH is $TARGETARCH"
RUN echo "KUBECTL_VERSION is $KUBECTL_VERSION"
RUN wget -O /usr/bin/kubectl "https://storage.googleapis.com/kubernetes-release/release/v$KUBECTL_VERSION/bin/linux/$TARGETARCH/kubectl" \
  && chmod +x /usr/bin/kubectl


FROM fedora:37 as build-converge
WORKDIR /build
RUN mkdir -p \
  /build/usr/bin
COPY --from=build-essentials /usr/bin/kubectl /build/usr/bin
COPY --from=build-ocpdoom /go/src/ocpvizdoom/ocpvizdoom /build/usr/bin


FROM fedora:37
RUN dnf update -y && dnf groupinstall 'Development Tools' -y \
  && dnf install -y \
  gcc-c++ make cmake SDL2-devel git zlib-devel bzip2-devel \
  libjpeg-turbo-devel fluidsynth-devel game-music-emu-devel openal-soft-devel \
  libmpg123-devel libsndfile-devel wildmidi-devel gtk3-devel timidity++ nasm tar \
  chrpath \
  cmake \
  git \
  boost-devel \ 
  SDL2-devel \
  openal-soft-devel \
  python3-devel \
  python3-pip \
  git-lfs \
  wget \
  xorg-x11-server-Xvfb \
  nmap \
  x11vnc \
  && dnf clean all
ADD . / ViZDoom/
WORKDIR /ViZDoom
RUN pip install . && mkdir train_dir && mkdir socket
RUN git clone       https://github.com/Syst3m1cAn0maly/sample-factory.git && cd sample-factory && pip install -e .
RUN cd /ViZDoom/train_dir && git lfs install && git clone https://huggingface.co/andrewzhang505/sample-factory-2-doom-battle
WORKDIR /ViZDoom
RUN wget https://github.com/Akbar30Bill/DOOM_wads/raw/master/doom2.wad 
COPY --from=build-converge /build /
RUN chgrp -R 0 /ViZDoom && \
  chmod -R g=u /ViZDoom && \
  mkdir /recs && chgrp -R 0 /recs && \
  chmod -R g=u /recs && \
  mkdir /.cache && chgrp -R 0 /.cache && \
  chmod -R g=u /.cache 
       

ENTRYPOINT ["/usr/bin/ocpvizdoom"]
