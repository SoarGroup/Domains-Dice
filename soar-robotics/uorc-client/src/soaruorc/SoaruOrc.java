package	soaruorc;

import orc.util.*;
import sml.*;
import java.io.Console;

public class SoaruOrc implements Kernel.UpdateEventInterface
{
	private GamePad gp;
	private uOrcThread uorc;
	private Kernel kernel;
	private Agent agent;
	private StringElement active;
	private FloatElement left;
	private FloatElement right;
	
	private boolean useGamePad = false;
	private boolean useRobot = false;
	
	public SoaruOrc()
	{
		if ( useGamePad )
		{
			gp = new GamePad();
		}
		
		if ( useRobot )
		{
			uorc = new uOrcThread();
		}
		
		kernel = Kernel.CreateKernelInNewThread();
		if ( kernel.HadError() )
		{
			System.err.println( kernel.GetLastErrorDescription() );
			System.exit(1);
		}

		agent = kernel.CreateAgent( "soar" );
		if ( kernel.HadError() )
		{
			System.err.println( kernel.GetLastErrorDescription() );
			System.exit(1);
		}
		
		// load productions
		agent.LoadProductions( "agents/simple-bot.soar" );
		
		// set up input link
		// override
		//     active true
		//         move
		//             left 0
		//             right 0
		Identifier override = agent.CreateIdWME( agent.GetInputLink(), "override" );
		active = agent.CreateStringWME( override, "active", "false" );
		Identifier move = agent.CreateIdWME( override, "move" );
		left = agent.CreateFloatWME( move, "left", 0 );
		right = agent.CreateFloatWME( move, "right", 0 );

		kernel.RegisterForUpdateEvent( smlUpdateEventId.smlEVENT_AFTER_ALL_GENERATED_OUTPUT, this, null );
		
		// start the bot thread
		if ( useRobot )
		{
			uorc.start();
		}
		
		System.out.printf( "%15s %15s %15s %15s\n", "left input", "right input", "left output", "right output" );
		
		// let the debugger debug
		Console console = System.console();
		while ( true )
		{
			String command = console.readLine();
			if ( command.equals( "quit" ) || command.equals( "exit" ) )
			{
				break;
			}
		}
			
		if ( useRobot )
		{
			uorc.stopThread();
		}
		kernel.Shutdown();
		kernel.delete();
		
		System.out.println( "Shutdown complete. Hit control-c to continue." );
	}

	public void updateEventHandler(int eventID, Object data, Kernel kernel, int runFlags) 
	{
		double leftInput = 0;
		double rightInput = 0;
		if ( useGamePad )
		{
			leftInput = gp.getAxis( 1 ) * -1;
			rightInput = gp.getAxis( 3 ) * -1;
		} 
		else
		{
			leftInput = Math.random();
			rightInput = Math.random();
		}
		
		// write input
		agent.Update( active, "true" );
		agent.Update( left, leftInput );
		agent.Update( right, rightInput );
		
		// process output
		double leftCommand = 0;
		double rightCommand = 0;
		for ( int i = 0; i < agent.GetNumberCommands(); ++i ) 
		{
			Identifier commandId = agent.GetCommand( i );
			String commandName = commandId.GetAttribute();
			
			if ( commandName.equals( "move" ) )
			{
				//System.out.print( "move: " );
				try 
				{
					leftCommand = Double.parseDouble( commandId.GetParameterValue( "left" ) );
				} 
				catch ( NullPointerException ex )
				{
					System.out.println( "No left on move command" );
					commandId.AddStatusError();
					continue;
				}
				catch ( NumberFormatException e ) 
				{
					System.out.println( "Unable to parse left: " + commandId.GetParameterValue( "left" ) );
					commandId.AddStatusError();
					continue;
				}
				//System.out.print( leftCommand + " " );
				
				try 
				{
					rightCommand = Double.parseDouble( commandId.GetParameterValue( "right" ) );
				} 
				catch ( NullPointerException ex )
				{
					System.out.println( "No right on move command" );
					commandId.AddStatusError();
					continue;
				}
				catch ( NumberFormatException e ) 
				{
					System.out.println( "Unable to parse right: " + commandId.GetParameterValue( "right" ) );
					commandId.AddStatusError();
					continue;
				}
				//System.out.print( rightCommand + " " );
				
				leftCommand = Math.max( leftCommand, -1.0 );
				leftCommand = Math.min( leftCommand, 1.0 );
				
				rightCommand = Math.max( rightCommand, -1.0 );
				rightCommand = Math.min( rightCommand, 1.0 );
				
				//System.out.print( leftCommand + " " );
				//System.out.print( rightCommand + "\n" );
				
				if ( useRobot )
				{
					uorc.setPower( leftCommand, rightCommand );
				}
				commandId.AddStatusComplete();
				continue;
			}
			
			System.out.println( "Unknown command: " + commandName );
			commandId.AddStatusError();
		}
		
		System.out.printf( "%15f %15f %15f %15f\r", leftInput, rightInput, leftCommand, rightCommand );
	}
	
	public static void main(String args[])
	{
		new SoaruOrc();
	}
}