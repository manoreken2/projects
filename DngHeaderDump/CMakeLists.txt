cmake_minimum_required(VERSION 3.8)

project(DngHeaderDump LANGUAGES CSharp)

include(CSharpUtilities)

add_executable(DngHeaderDump
	Program.cs)

# This project uses DngRW library
target_link_libraries(DngHeaderDump DngRW)
	
set_property(TARGET DngHeaderDump PROPERTY VS_DOTNET_REFERENCES
	"Microsoft.CSharp"
	"System"
	"System.Core"
	"WindowsBase")
