arr=`ls examples`
set -e
for i in $arr; do
    echo "# running "$i"...";
    ./ciberia ./examples/$i
done
