::JDK 7
@set JAVA_HOME=D:\Program Files\Java\jdk1.7.0_71
@set path=D:\Program Files\Java\jdk1.7.0_71\bin;%path%

::Android NDK
@set path=%path%;F:\pan1\android-ndk-r9c
@set path=%path%;E:\android-ndk-r10e
@set path=%path%;D:\android-ndk-r9c

::Android SDK
@set path=%path%;D:\adt-bundle-windows-x86-20140624\sdk\platform-tools
@set path=%path%;D:\adt-bundle-windows-x86-20140624\sdk\tools

::Apache Ant
@set path=%path%;D:\adt-bundle-windows-x86-20140624\apache-ant-1.9.4\bin

::see avd manager
::@set ANDROID_SDK_HOME=D:\Users\Administrator
::@set ANDROID_SDK_HOME=C:\Documents and Settings\Administrator

@cmd

:: run 'ndk-build' or
:: run 'ndk-build clean all'
