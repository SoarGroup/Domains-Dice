package org.msoar.sps.sm;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.net.Socket;
import java.util.ArrayList;

import org.apache.log4j.Logger;
import org.msoar.sps.Names;

public class RemoteRunner implements Runner {
	private static Logger logger = Logger.getLogger(RemoteRunner.class);
	
	private String component;
	private ObjectOutputStream oout;
	private ObjectInputStream oin;
	private Boolean aliveResponse;
	
	RemoteRunner(Socket socket) throws IOException {
		this.oout = new ObjectOutputStream(new BufferedOutputStream(socket.getOutputStream()));
		this.oout.flush();
		
		this.oin = new ObjectInputStream(new BufferedInputStream(socket.getInputStream()));
		
		logger.debug("new remote runner waiting for component name");
		this.component = NetworkRunner.readString(oin);
		if (component == null) {
			throw new IOException();
		}

		logger.debug("'" + component + "' received, writing ok");
		oout.writeObject(Names.NET_OK);
		this.oout.flush();
	}

	@Override
	public void configure(ArrayList<String> command, String config) throws IOException {
		oout.writeObject(Names.NET_CONFIGURE);
		oout.writeObject(command);
		if (config == null) {
			oout.writeObject(Names.NET_CONFIG_NO);
		} else {
			oout.writeObject(Names.NET_CONFIG_YES);
			oout.writeObject(config);
		}
		oout.flush();
	}

	@Override
	public void stop() throws IOException {
		oout.writeObject(Names.NET_STOP);
		oout.flush();
	}

	@Override
	public void quit() {
		try {
			oout.writeObject(Names.NET_QUIT);
			oout.flush();
			oout.close();
			oin.close();
		} catch (IOException ignored) {
		}
	}

	@Override
	public String getComponentName() {
		return component;
	}

	@Override
	public boolean isAlive() throws IOException {
		aliveResponse = null;
		oout.writeObject(Names.NET_ALIVE);
		oout.flush();
		aliveResponse = NetworkRunner.readBoolean(oin);

		if (aliveResponse == null) {
			throw new IOException();
		}
		
		return aliveResponse;
	}

	@Override
	public void start() throws IOException {
		oout.writeObject(Names.NET_START);
		oout.flush();
	}

	private class OutputPump implements Runnable {
		BufferedReader output;
		
		OutputPump(BufferedReader output) {
			this.output = output;
		}
		
		@Override
		public void run() {
			logger.debug(component + ": output pump alive");
			String out;
			try {
				while (( out = output.readLine()) != null) {
					System.out.println(out);
				}
			} catch (IOException e) {
				logger.error(e.getMessage());
			}
		}
	}
	
	@Override
	public void setOutput(BufferedReader output) {
		Thread thread = new Thread(new OutputPump(output));
		thread.setDaemon(true);
		thread.start();
	}
}