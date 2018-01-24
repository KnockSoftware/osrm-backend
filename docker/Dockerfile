FROM alpine:3.6 as buildstage

ARG DOCKER_TAG
RUN mkdir -p /src  && mkdir -p /opt
COPY . /src
WORKDIR /src

RUN NPROC=$(grep -c ^processor /proc/cpuinfo 2>/dev/null || 1) && \
    echo "@testing http://dl-cdn.alpinelinux.org/alpine/edge/testing" >> /etc/apk/repositories && \
    apk update && \
    apk upgrade && \
    apk add nodejs nodejs-npm git cmake wget make libc-dev gcc g++ bzip2-dev boost-dev zlib-dev expat-dev lua5.2-dev libtbb@testing libtbb-dev@testing && \
    npm install --ignore-scripts && \
    NPROC=$(grep -c ^processor /proc/cpuinfo 2>/dev/null || 1) && \
    echo "Building OSRM ${DOCKER_TAG}" && \
    git show --format="%H" | head -n1 > /opt/OSRM_GITSHA && \
    echo "Building OSRM gitsha $(cat /opt/OSRM_GITSHA)" && \
    mkdir -p build && \
    cd build && \
    BUILD_TYPE="Release" && \
    ENABLE_ASSERTIONS="Off" && \
    BUILD_TOOLS="Off" && \
    case ${DOCKER_TAG} in *"-debug"*) BUILD_TYPE="Debug";; esac && \
    case ${DOCKER_TAG} in *"-assertions"*) BUILD_TYPE="RelWithDebInfo" && ENABLE_ASSERTIONS="On" && BUILD_TOOLS="On";; esac && \
    echo "Building ${BUILD_TYPE} with ENABLE_ASSERTIONS=${ENABLE_ASSERTIONS} BUILD_TOOLS=${BUILD_TOOLS}" && \
    cmake .. -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DENABLE_ASSERTIONS=${ENABLE_ASSERTIONS} -DENABLE_NODE_BINDINGS=On -DBUILD_TOOLS=${BUILD_TOOLS} -DENABLE_LTO=On && \
    make -j${NPROC} install
    
RUN \
    npm pack && \
    cd ./profiles && \
    cp -r * /opt && \
    \
    strip /usr/local/bin/* && \
    rm -rf /src


# Multistage build to reduce image size - https://docs.docker.com/engine/userguide/eng-image/multistage-build/#use-multi-stage-builds
# Only the content below ends up in the image, this helps remove /src from the image (which is large)
FROM alpine:3.6 as runstage
RUN mkdir -p /src  && mkdir -p /opt
RUN echo "@testing http://dl-cdn.alpinelinux.org/alpine/edge/testing" >> /etc/apk/repositories && \
    apk update && \
    apk add nodejs nodejs-npm boost-filesystem boost-program_options boost-regex boost-iostreams boost-thread libgomp lua5.2 expat libtbb@testing
COPY --from=buildstage /usr/local /usr/local
COPY --from=buildstage /opt /opt
COPY --from=buildstage /src/osrm-*.tgz /opt
RUN npm install -g /opt/osrm-*.tgz
WORKDIR /opt

EXPOSE 5000
