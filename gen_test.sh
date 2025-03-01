#!/usr/bin/bash
echo "Running dlt generation Sample"
current=$(PWD)
python3 $(current)/test/external_tools/pydlt/main.py
echo "--- Finished ---"