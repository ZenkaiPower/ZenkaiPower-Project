## ZENKAI Power! Docker image

Login on Docker Hub.
```powershell
docker login
```

Build the image.
```powershell
docker build --platform linux/amd64 -t zenkaipower/zenkaipower:latest .
```

Push image.
```powershell
docker push zenkaipower/zenkaipower:latest
```

Image available at: https://hub.docker.com/r/zenkaipower/zenkaipower

Download the image.
```powershell
docker pull zenkaipower/zenkaipower:latest
```

Deleting the image.
```powershell
docker rmi -f zenkaipower:latest
```
