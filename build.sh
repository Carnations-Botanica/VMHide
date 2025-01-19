xcodebuild clean
xcodebuild -jobs 1 -configuration Debug
xcodebuild -jobs 1 -configuration Release
open build/Debug