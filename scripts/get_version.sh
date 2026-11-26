#!/bin/bash
sed -n 's/.*FW_VERSION[[:space:]]*"\([0-9.]*\)".*/\1/p' app/version.h
