#!/bin/bash
# Usage: . ./run_alcapana.sh XXXX
# XXXX = run number

RUN_NUMBER=$1

odbedit -c "load run${RUN_NUMBER}.odb"
./alcapana -i run${RUN_NUMBER}.mid -o hist${RUN_NUMBER}.root -T tree${RUN_NUMBER}.root