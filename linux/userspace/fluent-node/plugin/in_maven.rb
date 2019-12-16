require 'fluent/input'
require 'time'

=begin
This is a fluent input module
Steps to install and use:
1. Install fluent using rubygems 'gem install fluentd'
	Also install the s3 plugin with 'gem install fluent-plugin-s3'
2. Place this file in the plugins directory /etc/fluentd/plugins/
3. Edit the /etc/fluentd/fluentd.conf file to include something like
<source>
	@type maven
	socket_file /var/run/maven.sock (optional)
</source>
4. Insert the maven module mod_log
5. Start fluentd logging using the fluentd command
6. Verify that calls to the kernel module symbol log_event are successfully
	pushed to the desired log output

# TODO
As of now, all events emited from this plugin are tagged with
debug.maven
In the future, we will probably use the maven.** tag namespace
=end
module Fluent
	class MavenInput < Input
		Fluent::Plugin.register_input('maven', self)
		config_param :socket_file, :string, :default => '/var/run/maven.sock'

		def configure( conf )
			super
			@sock_fn = conf['socket_file']
			@TagPrefix = "maven."
		end

		def parse_msg( msgstr )
			rest = msgstr
			ret = {}
			while( not( rest.nil?) && rest != "" ) do
				key,type,rest=rest.split( ":", limit=3 )
				if( type == "i" )
					val,delim = rest.unpack("Qc")
					if( delim != ';'.ord )
						raise "bad delim #{delim} on key #{key}"
					end
					rest = rest[9..-1] # discard the 8 byte int and 1 byte delim
					ret[key] = val
				elsif ( type == "s" )
					val,rest = rest.split( ";", limit=2 )
					ret[key]=val
				elsif (type == "b" )
					size = rest.unpack( "Q" )[0]
					rest = rest[8..-1] # discard the uint64
					if( rest.length <= size )
						raise "not enought characters for promised bytestring size of #{size}"
					end
					bytes,delim = rest.unpack( "H#{size*2}c" )
					if( delim != ';'.ord )
						raise "bad delim #{delim} on key #{key}"
					end
					ret[key] = bytes
					rest = rest[size+1..-1] # drop the bytestring and delim
				elsif (type == "u" )
					# unpack 32 nibbles (16 bytes, 128 bits )
					ret[key] = rest.unpack( "H32" )
					rest = rest[16..-1]
				else
					raise "Unknown type #{type} on key #{key}"
				end
			end
			return ret
		end

		def start
			super
			File.delete( @sock_fn ) if File.exists?( @sock_fn )
			begin
				@serv = UNIXServer.new( @sock_fn )
			rescue
				router.emit(@TagPrefix + "logger", Engine.now, {"message"=>"unable to start unixfile server"} )
				return
			end
			router.emit(@TagPrefix + "logger", Engine.now, {"message"=>"unixfile server started"} )
			puts( "Starting maven log listener thread" )
			Thread.new do
				loop do
					Thread.start(@serv.accept) do |s|
						p = s.read
						if (p == nil )
							break
						end
						begin
							parsed = parse_msg( p )
						rescue Exception => e
							puts( "I guess we got #{e}" )
							puts( e.backtrace )
						end
						puts( "got message #{parsed}" )
						router.emit(@TagPrefix + "logger", Engine.now, parsed )
						s.close
					end
				end
			end
			#open unix socket
			#spawn watcher thread
		end

		def shutdown
			super
			@serv.close if (@serv)
		end
	end
end
