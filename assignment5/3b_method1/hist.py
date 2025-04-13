# -*- coding: utf-8 -*-
# code to plot histograms of different methods of pin toggleing 
# not original work generated with LLM 
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

# Set Seaborn theme
sns.set_theme(style="darkgrid")

# File names and corresponding labels
files = {
    "service_runs1_.csv": "Run 1",
    "service_runs2_.csv": "Run 2",
    "service_runs3_.csv": "Run 3",
    "service_runs4_.csv": "Run 4"
}

# Load and combine data
all_data = []
for file, label in files.items():
    df = pd.read_csv(file, header=None, names=["run_time"])
    df["run_label"] = label
    all_data.append(df)

combined_df = pd.concat(all_data, ignore_index=True)

# Create subplots (4 rows and 1 column)
fig, axes = plt.subplots(2, 2, figsize=(12, 10))  # Adjust the size as needed
axes = axes.flatten()  # Flatten the 2D array of axes for easier indexing

# Plot each histogram on a separate subplot
for i, label in enumerate(combined_df["run_label"].unique()):
    ax = axes[i]  # Get the correct axis for this plot
    subset = combined_df[combined_df["run_label"] == label]
    
    sns.histplot(
        data=subset,
        x="run_time",
        kde=True,  # No KDE, just histograms
        stat="count",  # Plot raw frequencies
        element="step",
        label=label,
        color=sns.color_palette()[i],
        ax=ax  # Plot on the specific axis
    )
    
    # Add vertical dashed line for variance visualization (mean Â± std)
    mean = subset["run_time"].mean()
    std = subset["run_time"].std()
    
    ax.axvline(mean, color=sns.color_palette()[i], linestyle="--", linewidth=1.5)
    ax.axvline(mean + std, color=sns.color_palette()[i], linestyle=":", linewidth=1)
    ax.axvline(mean - std, color=sns.color_palette()[i], linestyle=":", linewidth=1)

    ax.set_title(f"Distribution for {label}")
    ax.set_xlabel("Run Time")
    ax.set_ylabel("Frequency")

# Adjust layout and show the plot
plt.tight_layout()
plt.show()

# Print summary stats
print("\nSummary Statistics (including variance):")
summary = combined_df.groupby("run_label")["run_time"].agg(['count', 'mean', 'std', 'var', 'min', 'max'])
print(summary)
