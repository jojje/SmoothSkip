OUTDIR = "Release"

def version
  `grep "define VERSION" SmoothSkip.h`.strip.split.last.gsub(/"/,'')
end
FN = "SmoothSkip-#{version}.zip"

def files
  a = `git ls-files`.gsub(/\\/,"/").split("\n").reject{|s| s =~ /gitignore/}
  a << "Release/SmoothSkip.dll"
  a
end

def zip(fn)
  ofn = [OUTDIR,fn].join("/")
  File.delete ofn if File.exists?(ofn)
  cmd = "7z a %s %s" % [ofn, files.join(" ")]
  system cmd
end

task :default => :package

task :package do
  zip FN
end

task :upload => :package do
  ofn = "~/www/files/#{FN}"
  sh "scp #{OUTDIR}/#{FN} ukub:~/www/files/"
  sh %|ssh ukub "chmod 644 #{ofn}"|
  sh %|ssh ukub "sha1sum #{ofn}"|
  sh %|ssh ukub "sha256sum #{ofn}"|
end

