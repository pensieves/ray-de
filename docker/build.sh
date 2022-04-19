UBUNTU_VER=${1:-20.04}
APT_PKG_LIST=${2:-docker/externals/apt-pkg-list.txt}
SSH_PVT_KEY=${3:-docker/externals/id_rsa}

if [[ ! -f $SSH_PVT_KEY ]]
then
    ssh-keygen -q -N "" -f $SSH_PVT_KEY
fi

docker build \
    --build-arg UBUNTU_VER=$UBUNTU_VER \
    --build-arg APT_PKG_LIST=$APT_PKG_LIST \
    --build-arg SSH_PVT_KEY=$SSH_PVT_KEY \
    --build-arg PIP_REQ_FILE=requirements.txt \
    -t ray-de \
    -f docker/Dockerfile .