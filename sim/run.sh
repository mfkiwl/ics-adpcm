#!/bin/sh

set -eu

./build.sh

for d in test/*/; do
	echo "Running tests for directory ${d}..."

	./obj_dir/Vics_adpcm \
		--test-config ${d}/config.json \
		--ch-config ${d}/channel.json \
		--adpcm-capture-reference ${d}/adpcm_reference.json \
		--wav-reference ${d}/reference.wav

	echo "...test completed\n"

done

