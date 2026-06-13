FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y \
	build-essential \
	cmake \
	ninja-build \
	python3 \
	python3-pip \
	git \
	&& rm -rf /var/lib/apt/lists/*

RUN pip3 install conan --break-system-packages

RUN conan profile detect --force && \
      sed -i 's/compiler.cppstd=gnu17/compiler.cppstd=20/' /root/.conan2/profiles/default

WORKDIR /app
COPY . .

RUN conan install . \
	--output-folder=build \
	--build=missing \
	-s build_type=Release

RUN cmake -S . -B build \
	-DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
	-DCMAKE_BUILD_TYPE=Release \
	-G Ninja

RUN cmake --build build

FROM ubuntu:24.04

WORKDIR /app

COPY --from=builder /app/build/simulador_ospf .
COPY --from=builder /app/data ./data

ENTRYPOINT ["./simulador_ospf"]
CMD ["data/standard.json"]
