cmake_minimum_required(VERSION 3.8)

project(ImageToDng LANGUAGES CSharp)

include(CSharpUtilities)

add_executable(ImageToDng
	App.xaml
	App.xaml.cs
	MainWindow.xaml
	MainWindow.xaml.cs
	Properties/AssemblyInfo.cs
	Properties/Resources.Designer.cs
	Properties/Resources.resx
	Properties/Settings.Designer.cs
	Properties/Settings.settings)

csharp_set_xaml_cs_properties(
	App.xaml
	App.xaml.cs
	MainWindow.xaml
	MainWindow.xaml.cs)

csharp_set_designer_cs_properties(
	Properties/AssemblyInfo.cs
	Properties/Resources.Designer.cs
	Properties/Resources.resx
	Properties/Settings.Designer.cs
	Properties/Settings.settings)

set_property(SOURCE App.xaml PROPERTY VS_XAML_TYPE "ApplicationDefinition")

# Windows app, not console app
set_property(TARGET ImageToDng PROPERTY WIN32_EXECUTABLE TRUE)	

# This project uses DngRW library
target_link_libraries(ImageToDng DngRW)

# lists dotnet libraries
set_property(TARGET ImageToDng PROPERTY VS_DOTNET_REFERENCES
	"Microsoft.CSharp"
	"PresentationCore"
	"PresentationFramework"
	"System"
	"System.Core"
	"System.Drawing"
	"System.Xaml"
	"WindowsBase")
