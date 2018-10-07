cmake_minimum_required(VERSION 3.8)

project(DngRW LANGUAGES CSharp)

include(CSharpUtilities)

add_library(DngRW SHARED
	DngReader.cs
	DngWriter.cs
	IFDEntry.cs
	IFDRational.cs)

set_property(TARGET DngRW PROPERTY VS_DOTNET_REFERENCES
	"Microsoft.CSharp"
	"System"
	"System.Core"
	"WindowsBase")
