echo "clang-format -i willow/src/*cpp"
clang-format -i willow/src/*cpp
echo "clang-format -i willow/src/gcipu/*cpp"
clang-format -i willow/src/*cpp
echo "clang-format -i willow/include/willow/*hpp"
clang-format -i willow/include/willow/*hpp
echo "clang-format -i willow/include/willow/gcipu/*hpp"
clang-format -i willow/include/willow/*hpp
echo "clang-format -i pywillow/*cpp"
clang-format -i pywillow/*cpp
echo "yapf -i tests/basic/*py"
yapf -i tests/basic/*py
echo "yapf -i pywillow/*py"
yapf -i pywillow/*py
