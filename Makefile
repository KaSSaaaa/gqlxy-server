format:
	@find include src tests samples -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) -exec clang-format -i --style=file {} \;
