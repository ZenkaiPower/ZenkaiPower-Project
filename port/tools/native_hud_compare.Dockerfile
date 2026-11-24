FROM zenkaipower:latest
RUN apt-get update -qq && apt-get install -y --no-install-recommends libgl1-mesa-dri xvfb xauth python3-pil && rm -rf /var/lib/apt/lists/*
