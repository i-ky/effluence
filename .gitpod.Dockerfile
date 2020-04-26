FROM gitpod/workspace-full
                    
USER gitpod

RUN sudo apt-get -q update && \
    sudo apt-get install -yq autoconf automake libcurl4-openssl-dev libyaml-dev bear && \
    sudo rm -rf /var/lib/apt/lists/*
