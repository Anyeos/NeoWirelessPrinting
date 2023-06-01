#!/bin/bash

rm w/*.gz
rm w/*.woff2

cp *.html w
cp *.js w
cp *.css w
cp *.woff2 w

cd w
gzip *.html *.js *.css

