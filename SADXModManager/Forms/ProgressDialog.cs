using System;
using System.Linq;
using System.Windows.Forms;

namespace SADXModManager.Forms
{
	public partial class ProgressDialog : Form
	{
		#region Accessors

		/// <summary>
		/// Gets or sets the current task displayed on the window. (Upper label)
		/// </summary>
		public string Task => labelTask.Text;

		/// <summary>
		/// Gets or sets the current step displayed on the window. (Lower label)
		/// </summary>
		public string Step => labelStep.Text;

		/// <summary>
		/// Gets or sets the title of the window.
		/// </summary>
		public string Title => Text;

		/// <summary>
		/// Gets or sets the enabled state of the close options.
		/// If true, the auto-close checkbox and OK button will be disabled.
		/// </summary>
		public bool EnableCloseOptions
		{
			get
			{
				return checkAutoClose.Checked;
			}
			set
			{
				checkAutoClose.Enabled = value;
				SetOkEnabledState();
			}
		}

		// HACK: Work around to avoid animation which is not configurable and FAR too slow.
		private int progressValue
		{
			get
			{
				return progressBar.Value;
			}
			set
			{
				++progressBar.Maximum;
				progressBar.Value = value + 1;
				progressBar.Value = value;
				--progressBar.Maximum;
			}
		}

		#endregion

		private int taskIndex;
		private int[] taskSteps;

		/// <summary>
		/// Initializes a ProgressDialog which displays the current task, the step in that task, and a progress bar.
		/// </summary>
		/// <param name="title">The title of the window</param>
		/// <param name="max">The number of steps required</param>
		/// <param name="enableCloseOptions">Defines whether or not the close-on-completion checkbox and OK button are usable.</param>
		/// <param name="autoClose">The default close-on-completion option.</param>
		public ProgressDialog(string title, int[] taskSteps, bool enableCloseOptions = false, bool autoClose = true)
		{
			InitializeComponent();

			if (taskSteps == null)
			{
				throw new ArgumentNullException(nameof(taskSteps));
			}

			if (taskSteps.Length < 1)
			{
				throw new ArgumentException("Array must not be empty.", nameof(taskSteps));
			}

			this.taskSteps = taskSteps;

			Text                   = title;
			EnableCloseOptions     = enableCloseOptions;
			checkAutoClose.Checked = autoClose;
			labelTask.Text         = "";
			labelStep.Text         = "";
			progressBar.Maximum    = taskSteps.Sum();
		}

		public void NextTask()
		{
			if (taskIndex + 1 < taskSteps.Length)
			{
				++taskIndex;
				int value = 0;

				for (int i = 0; i < taskIndex; i++)
				{
					value += taskSteps[i];
				}

				progressValue = value;
				return;
			}

			progressBar.Value = progressBar.Maximum;
			Finish();
		}

		/// <summary>
		/// Increments the progress bar.
		/// </summary>
		/// <param name="amount">The amount by which to increment. Defaults to 1.</param>
		public void StepProgress(int amount = 1)
		{
			if (InvokeRequired)
			{
				Invoke((Action<int>)StepProgress, amount);
				return;
			}

			// Not using progressBar.Step() because dirty hacks
			progressValue = progressValue + amount;

			if (!EnableCloseOptions || progressBar.Value != progressBar.Maximum)
			{
				return;
			}

			Finish();
		}

		public void SetProgress(int value)
		{
			if (InvokeRequired)
			{
				Invoke((Action<int>)SetProgress, value);
				return;
			}

			progressValue = value;
		}

		private void Finish()
		{
			if (!EnableCloseOptions)
			{
				return;
			}

			if (checkAutoClose.Checked)
			{
				Close();
			}
			else
			{
				buttonOK.Enabled = true;
			}
		}

		/// <summary>
		/// Sets the current task to display on the window. (Upper label)
		/// </summary>
		/// <param name="text">The string to display as the task.</param>
		public void SetTask(string text)
		{
			if (InvokeRequired)
			{
				Invoke((Action<string>)SetTask, text);
			}
			else
			{
				labelTask.Text = text;
			}
		}

		/// <summary>
		/// Sets the current step to display on the window. (Lower label)
		/// </summary>
		/// <param name="text">The string to display as the step.</param>
		public void SetStep(string text)
		{
			if (InvokeRequired)
			{
				Invoke((Action<string>)SetStep, text);
			}
			else
			{
				labelStep.Text = text;
			}
		}

		/// <summary>
		/// Sets the task and step simultaneously.
		/// Both parameters default to null, so you may also use this to clear them.
		/// </summary>
		/// <param name="task">The string to display as the task. (Upper label)</param>
		/// <param name="step">The string to display as the step. (Lower label)</param>
		public void SetTaskAndStep(string task = null, string step = null)
		{
			if (InvokeRequired)
			{
				Invoke((Action<string, string>)SetTaskAndStep, task, step);
			}
			else
			{
				labelTask.Text = task;
				labelStep.Text = step;
			}
		}

		private void SetOkEnabledState()
		{
			if (progressBar.Value < progressBar.Maximum)
			{
				buttonOK.Enabled = !checkAutoClose.Checked;
			}
		}

		private void checkAutoClose_CheckedChanged(object sender, EventArgs e)
		{
			SetOkEnabledState();
		}

		private void ProgressDialog_Load(object sender, EventArgs e)
		{
			CenterToParent();
			buttonOK.Select();
		}
		private void buttonOK_Click(object sender, EventArgs e)
		{
			Close();
		}
	}
}
