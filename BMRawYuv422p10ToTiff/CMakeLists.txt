cmake_minimum_required(VERSION 3.8)

project(BMRawYuv422p10ToTiff LANGUAGES CSharp)

include(CSharpUtilities)

add_executable(BMRawYuv422p10ToTiff
	Properties/AssemblyInfo.cs
	Convert.cs
	Program.cs)

csharp_set_designer_cs_properties(
	Properties/AssemblyInfo.cs)

# This project uses DngRW library
target_link_libraries(BMRawYuv422p10ToTiff DngRW)

set_property(TARGET BMRawYuv422p10ToTiff PROPERTY VS_DOTNET_REFERENCES
	"Microsoft.CSharp"
	"System"
	"System.Core"
	"WindowsBase")
