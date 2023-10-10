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
  xorg-x11-server-Xvfb \
  nmap \
  x11vnc \
  && dnf clean all
ADD . / ViZDoom/
WORKDIR /ViZDoom
RUN pip install .
RUN pip install sample-factory

ENTRYPOINT ["tail", "-f", "/dev/null"]
