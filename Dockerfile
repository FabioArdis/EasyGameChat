FROM golang:1.27-alpine AS build

WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download

COPY . .
RUN CGO_ENABLED=0 go build -trimpath -ldflags="-s -w" -o /out/easygamechat .

FROM alpine:3.24

LABEL org.opencontainers.image.source="https://github.com/FabioArdis/EasyGameChat"
LABEL org.opencontainers.image.description="EasyGameChat server"
LABEL org.opencontainers.image.licenses="MIT"

RUN adduser -D -H -u 10001 easygamechat

WORKDIR /app
COPY --from=build /out/easygamechat /usr/local/bin/easygamechat

USER easygamechat
EXPOSE 3000

ENTRYPOINT ["easygamechat"]
CMD ["--listen", ":3000", "--cert", "/certs/server.crt", "--key", "/certs/server.key"]
