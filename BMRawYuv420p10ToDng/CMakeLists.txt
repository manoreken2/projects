cmake_minimum_required(VERSION 3.8)

project(BMRawYuv420p10ToDng LANGUAGES CSharp)

include(CSharpUtilities)

add_executable(BMRawYuv420p10ToDng
	Convert.cs
	Program.cs)

# This project uses DngRW library
target_link_libraries(BMRawYuv420p10ToDng DngRW)
	
set_property(TARGET BMRawYuv420p10ToDng PROPERTY VS_DOTNET_REFERENCES
	"Microsoft.CSharp"
	"System"
	"System.Core"
	"WindowsBase")
