#!/bin/bash

for i in {200..10000}0
do

	echo $i $(./a.out $i BIBA 0 WIDE1 1 \>test 1.wav)
done
