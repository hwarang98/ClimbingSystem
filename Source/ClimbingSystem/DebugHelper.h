#pragma once

namespace Debug
{
	static void Print(const FString& Message, const FColor& Color = FColor::MakeRandomColor(), int InKey = -1)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(InKey, 6.f, Color, Message);
		}

		UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
	}
}
