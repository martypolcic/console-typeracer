# Stage 1: Build C binaries
FROM alpine:3.20 AS builder

RUN apk add --no-cache cmake make gcc musl-dev

WORKDIR /src
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build

# Stage 2: Minimal runtime image
FROM alpine:3.20 AS runtime

RUN apk add --no-cache nodejs npm

WORKDIR /app

# Copy binaries and assets
COPY --from=builder /src/build/server/server /app/server
COPY --from=builder /src/build/client/client /app/client
COPY --from=builder /src/input.txt /app/input.txt

# Copy gateway and install production dependencies
COPY gateway /app/gateway
RUN cd /app/gateway && npm install --omit=dev

# Copy entrypoint
COPY entrypoint.sh /app/entrypoint.sh
RUN chmod +x /app/entrypoint.sh

ENV PORT=8080
ENV TCP_PORT=54897

EXPOSE 8080 54897

ENTRYPOINT ["/app/entrypoint.sh"]
