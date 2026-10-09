# libromm Build Agent

Docker-based Jenkins inbound agent for compiling libromm and native AmigaOS 3.x applications. This document is maintained in English for the 1.1 development branch.

## Included tools

- Jenkins inbound agent (Java 21)
- GCC, G++, Make, Git and Linux build utilities
- libcurl development libraries
- AmigaOS m68k cross-compiler, built from [AmigaPorts/m68k-amigaos-gcc](https://github.com/AmigaPorts/m68k-amigaos-gcc)

The Amiga toolchain is installed in `/opt/amiga`; `/opt/amiga/bin` is added to `PATH` automatically.

## Build the Docker image

From the repository root:

```sh
docker build -f extras/buildagent/Dockerfile -t libromm-build-agent:1.1 extras/buildagent
```

Building the cross-compiler may take a considerable amount of time.

## Configure Jenkins

Create a Jenkins agent with these settings:

- **Type:** Permanent Agent
- **Launch method:** Inbound Agent
- **Remote root directory:** `/home/jenkins/agent`

Set the agent name and connection parameters to match your Jenkins controller.

## Run the agent

Example for a Jenkins controller with WebSocket support:

```sh
docker run -d \
  --name libromm-build-agent \
  --restart unless-stopped \
  -e JENKINS_URL=https://jenkins.example.org/ \
  -e JENKINS_AGENT_NAME=libromm-build-agent \
  -e JENKINS_SECRET=YOUR_AGENT_SECRET \
  libromm-build-agent:1.1
```

- `JENKINS_URL`: Jenkins controller URL.
- `JENKINS_AGENT_NAME`: name of the agent configured in Jenkins.
- `JENKINS_SECRET`: inbound-agent connection secret. Never commit this value to Git.

For Jenkins connections over WebSocket, configure the inbound agent's WebSocket option according to the controller and agent-launch setup.

## Builds

The agent can build Linux and AmigaOS m68k binaries when the corresponding Makefiles and dependencies are configured for each target.

## Security

- Never store Jenkins secrets in the repository.
- Connect the agent only to trusted Jenkins controllers.
- Update container images regularly.
