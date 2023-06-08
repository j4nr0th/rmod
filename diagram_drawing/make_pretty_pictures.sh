#!/bin/bash

for file in *; do
  if [ "${file: -4}" == ".dot" ]; then
    dot -Tsvg "${file}" > "${file%".dot"}.svg"
  fi
done
