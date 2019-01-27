cmake_minimum_required(VERSION 3.8)

project(DumpAVIHeaders LANGUAGES CSharp)

include(CSharpUtilities)

add_executable(DumpAVIHeaders
	Properties/AssemblyInfo.cs
	Program.cs)

csharp_set_designer_cs_properties(
	Properties/AssemblyInfo.cs)

set_property(TARGET DumpAVIHeaders PROPERTY VS_DOTNET_REFERENCES
	"Microsoft.CSharp"
	"System"
	"System.Core"
	"WindowsBase")
