#!/bin/bash
git clone https://gitlab.com/akihe/radamsa.git
cd radamsa && git pull && make && sudo make install
