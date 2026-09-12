FROM gcc:14-bookworm AS build

WORKDIR /src
COPY backend/main.cpp backend/main.cpp
RUN g++ -std=c++17 -O2 -pthread backend/main.cpp -o /out/smartbill_server

FROM debian:bookworm-slim

WORKDIR /app
COPY --from=build /out/smartbill_server /app/smartbill_server
COPY assets ./assets
COPY includes ./includes
COPY modules ./modules
COPY pages ./pages
COPY index.html ./index.html

# Render supplies PORT at runtime. The database directory can be mounted to a
# persistent disk at /var/data without changing application code.
ENV SMARTBILL_DB_DIR=/var/data
EXPOSE 8080

CMD ["/app/smartbill_server"]
