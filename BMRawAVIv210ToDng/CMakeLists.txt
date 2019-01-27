cmake_minimum_required(VERSION 3.8)

project(BMRawAVIv210ToDng LANGUAGES CSharp)

include(CSharpUtilities)

add_executable(BMRawAVIv210ToDng
	Properties/AssemblyInfo.cs
	Convert.cs
	AviReader.cs
	Common.cs
	RiffHeader.cs
	ListHeader.cs
	BitmapInfoHeader.cs
	AviMainHeader.cs
	AviStreamHeader.cs
	FourCCHeader.cs
	Program.cs)

csharp_set_designer_cs_properties(
	Properties/AssemblyInfo.cs)

# This project uses DngRW library
target_link_libraries(BMRawAVIv210ToDng DngRW)

set_property(TARGET BMRawAVIv210ToDng PROPERTY VS_DOTNET_REFERENCES
	"Microsoft.CSharp"
	"System"
	"System.Core"
	"WindowsBase")
