#!/bin/bash

if [ "$(dpkg -l | grep "jdk")" == "" ]; then
	wget https://download.oracle.com/java/17/archive/jdk-17.0.10_linux-x64_bin.deb
	sudo dpkg -i jdk-17.0.10_linux-x64_bin.deb
	rm -f jdk-17.0.10_linux-x64_bin.deb
fi

if [ "$(dpkg -l | grep " gh ")" == "" ]; then
	sudo mkdir -p -m 755 /etc/apt/keyrings && wget -qO- https://cli.github.com/packages/githubcli-archive-keyring.gpg | sudo tee /etc/apt/keyrings/githubcli-archive-keyring.gpg > /dev/null
	sudo chmod go+r /etc/apt/keyrings/githubcli-archive-keyring.gpg
	echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | sudo tee /etc/apt/sources.list.d/github-cli.list > /dev/null
	sudo apt update
	sudo apt install gh -y
fi

if [ "$(dpkg -l | grep " jq ")" == "" ]; then
	sudo apt install jq -y
fi

if [ "$(dpkg -l | grep "cgroup-tools")" == "" ]; then
	sudo apt install cgroup-tools intel-cmt-cat
fi

if [ "$(dpkg -l | grep "docker")" == "" ]; then
	# Add Docker's official GPG key:
	sudo apt update
	sudo apt install ca-certificates curl
	sudo install -m 0755 -d /etc/apt/keyrings
	sudo curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
	sudo chmod a+r /etc/apt/keyrings/docker.asc

	# Add the repository to Apt sources:
	echo \
		"deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu \
		$(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
		sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
	sudo apt update
	
	sudo apt install docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

	sudo usermod -aG docker $USER
	newgrp docker
fi

