namespace SADXModManager
{
	partial class ModUpdatesDialog
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose(bool disposing)
		{
			if (disposing && (components != null))
			{
				components.Dispose();
			}
			base.Dispose(disposing);
		}

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.label1 = new System.Windows.Forms.Label();
			this.buttonInstall = new System.Windows.Forms.Button();
			this.buttonLater = new System.Windows.Forms.Button();
			this.listModUpdates = new System.Windows.Forms.ListView();
			this.columnName = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnVersion = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnNewVersion = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnDate = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnSize = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.SuspendLayout();
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(12, 9);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(169, 13);
			this.label1.TabIndex = 0;
			this.label1.Text = "The following mods have updates:";
			// 
			// buttonInstall
			// 
			this.buttonInstall.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.buttonInstall.FlatStyle = System.Windows.Forms.FlatStyle.System;
			this.buttonInstall.Location = new System.Drawing.Point(537, 246);
			this.buttonInstall.Name = "buttonInstall";
			this.buttonInstall.Size = new System.Drawing.Size(75, 23);
			this.buttonInstall.TabIndex = 3;
			this.buttonInstall.Text = "&Install now";
			this.buttonInstall.UseVisualStyleBackColor = true;
			this.buttonInstall.Click += new System.EventHandler(this.buttonInstall_Click);
			// 
			// buttonLater
			// 
			this.buttonLater.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.buttonLater.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.buttonLater.FlatStyle = System.Windows.Forms.FlatStyle.System;
			this.buttonLater.Location = new System.Drawing.Point(456, 246);
			this.buttonLater.Name = "buttonLater";
			this.buttonLater.Size = new System.Drawing.Size(75, 23);
			this.buttonLater.TabIndex = 2;
			this.buttonLater.Text = "Install &later";
			this.buttonLater.UseVisualStyleBackColor = true;
			// 
			// listModUpdates
			// 
			this.listModUpdates.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.listModUpdates.CheckBoxes = true;
			this.listModUpdates.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.columnName,
            this.columnVersion,
            this.columnNewVersion,
            this.columnDate,
            this.columnSize});
			this.listModUpdates.FullRowSelect = true;
			this.listModUpdates.HideSelection = false;
			this.listModUpdates.Location = new System.Drawing.Point(12, 25);
			this.listModUpdates.Name = "listModUpdates";
			this.listModUpdates.Size = new System.Drawing.Size(600, 215);
			this.listModUpdates.TabIndex = 1;
			this.listModUpdates.UseCompatibleStateImageBehavior = false;
			this.listModUpdates.View = System.Windows.Forms.View.Details;
			this.listModUpdates.ItemChecked += new System.Windows.Forms.ItemCheckedEventHandler(this.listModUpdates_ItemChecked);
			// 
			// columnName
			// 
			this.columnName.Text = "Name";
			this.columnName.Width = 226;
			// 
			// columnVersion
			// 
			this.columnVersion.Text = "Version";
			// 
			// columnNewVersion
			// 
			this.columnNewVersion.Text = "New Version";
			this.columnNewVersion.Width = 76;
			// 
			// columnDate
			// 
			this.columnDate.Text = "Release Date";
			this.columnDate.Width = 153;
			// 
			// columnSize
			// 
			this.columnSize.Text = "Size";
			// 
			// ModUpdatesDialog
			// 
			this.AcceptButton = this.buttonInstall;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.buttonLater;
			this.ClientSize = new System.Drawing.Size(624, 281);
			this.Controls.Add(this.listModUpdates);
			this.Controls.Add(this.buttonLater);
			this.Controls.Add(this.buttonInstall);
			this.Controls.Add(this.label1);
			this.MinimumSize = new System.Drawing.Size(320, 320);
			this.Name = "ModUpdatesDialog";
			this.ShowIcon = false;
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Mod Updates Available";
			this.Load += new System.EventHandler(this.ModUpdatesDialog_Load);
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Button buttonInstall;
		private System.Windows.Forms.Button buttonLater;
		private System.Windows.Forms.ListView listModUpdates;
		private System.Windows.Forms.ColumnHeader columnName;
		private System.Windows.Forms.ColumnHeader columnVersion;
		private System.Windows.Forms.ColumnHeader columnNewVersion;
		private System.Windows.Forms.ColumnHeader columnDate;
		private System.Windows.Forms.ColumnHeader columnSize;
	}
}