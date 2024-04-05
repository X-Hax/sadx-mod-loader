#!/bin/bash

if ! command -v zip &> /dev/null; then
    echo "zip command not found. Installing zip..."
    # Install zip using pacman
    pacman -S zip
fi


# Specify the directory containing headers
loaderDirectory="SADXModLoader/include"
commonDirectory="mod-loader-common/ModLoaderCommon"
libmodDir="libmodutils"

# Use find to locate all header files in the specified directory
headers=($(find "$loaderDirectory" -type f -name "*.h"))

# Add headers from other projects
headers+=("$commonDirectory/IniFile.hpp" "$commonDirectory/IniFile.cpp",  "$commonDirectory/Utils.hpp", "$commonDirectory/TextConv.hpp, $commonDirectory/TextConv.cpp")

files=($(find "$libmodDir" \( -name "*.cpp" -or -name "*.h" \) -and -not -name "stdafx.*"))

# Combine the headers and files arrays
all_files=("${headers[@]}" "${files[@]}")

# Create a temporary directory
temp_dir=$(mktemp -d)

# Loop through all files and copy them to the temporary directory
for file in "${all_files[@]}"; do
  cp "$file" "$temp_dir"
done

# Create a zip file
Compress-Archive -LiteralPath sadx_programming.zip "$temp_dir"

# Clean up temporary directory
rm -r "$temp_dir"

# Prompt user to press Enter before closing the window
read -p "Press Enter to exit"