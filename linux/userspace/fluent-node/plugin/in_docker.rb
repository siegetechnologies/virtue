require 'fluent/input'
require 'docker'
require 'pp'



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
=end
module Fluent
	class DockerInput < Input
	
		Fluent::Plugin.register_input('docker', self)

		def configure( conf )
			super
		end
			
		def start
			super
			puts( "Starting docker thread" )
			Thread.new do 
				loop do
					d = Docker::Container.all()
					if( d.empty? )
						puts( "No docker containers" )
					else
						d.each do |d|
							router.emit( "debug.docker", Engine.now, d.stats )
						end
					end
					sleep( 5 )
				end
			end
		end

		def shutdown
			super
		end
	end
end
